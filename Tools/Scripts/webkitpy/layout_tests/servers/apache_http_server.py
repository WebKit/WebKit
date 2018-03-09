# Copyright (C) 2015 Apple Inc. All rights reserved.
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


import logging
import os
import re
import socket

from webkitpy.layout_tests.servers import http_server_base


_log = logging.getLogger(__name__)


class LayoutTestApacheHttpd(http_server_base.HttpServerBase):
    def __init__(self, port_obj, output_dir, additional_dirs=None, port=None):
        """Args:
          port_obj: handle to the platform-specific routines
          output_dir: the absolute path to the layout test result directory
        """
        http_server_base.HttpServerBase.__init__(self, port_obj)
        # We use the name "httpd" instead of "apache" to make our paths (e.g. the pid file: /tmp/WebKit/httpd.pid)
        # match old-run-webkit-tests: https://bugs.webkit.org/show_bug.cgi?id=63956

        self._name = 'httpd'
        self._port = port
        if self._port is not None:
            self._mappings = [{'port': self._port}]
        else:
            self._mappings = [{'port': self.HTTP_SERVER_PORT},
                              {'port': self.ALTERNATIVE_HTTP_SERVER_PORT},
                              {'port': self.HTTPS_SERVER_PORT, 'sslcert': True}]
        self._output_dir = output_dir
        self._filesystem.maybe_make_directory(output_dir)

        self._pid_file = self._filesystem.join(self._runtime_path, '%s.pid' % self._name)

        if port_obj.host.platform.is_cygwin():
            # Convert to MSDOS file naming:
            precompiledBuildbot = re.compile('^/home/buildbot')
            precompiledDrive = re.compile('^/cygdrive/[cC]')
            output_dir = precompiledBuildbot.sub("C:/cygwin/home/buildbot", output_dir)
            output_dir = precompiledDrive.sub("C:", output_dir)
            self.tests_dir = precompiledBuildbot.sub("C:/cygwin/home/buildbot", self.tests_dir)
            self.tests_dir = precompiledDrive.sub("C:", self.tests_dir)
            self._pid_file = self._filesystem.join("C:/xampp/apache/logs", '%s.pid' % self._name)

        mime_types_path = self._filesystem.join(self.tests_dir, "http", "conf", "mime.types")
        cert_file = self._filesystem.join(self.tests_dir, "http", "conf", "webkit-httpd.pem")
        access_log = self._filesystem.join(output_dir, "access_log.txt")
        error_log = self._filesystem.join(output_dir, "error_log.txt")
        document_root = self._filesystem.join(self.tests_dir, "http", "tests")
        php_ini_dir = self._filesystem.join(self.tests_dir, "http", "conf")

        if port_obj.get_option('http_access_log'):
            access_log = port_obj.get_option('http_access_log')

        if port_obj.get_option('http_error_log'):
            error_log = port_obj.get_option('http_error_log')

        # FIXME: We shouldn't be calling a protected method of _port_obj!
        executable = self._port_obj._path_to_apache()
        config_file_path = self._copy_apache_config_file(self.tests_dir, output_dir)

        start_cmd = [executable,
            '-f', config_file_path,
            '-C', 'DocumentRoot "%s"' % document_root,
            '-c', 'TypesConfig "%s"' % mime_types_path,
            '-c', 'PHPINIDir "%s"' % php_ini_dir,
            '-c', 'CustomLog "%s" common' % access_log,
            '-c', 'ErrorLog "%s"' % error_log,
            '-c', 'PidFile "%s"' % self._pid_file,
            '-k', "start"]

        for (alias, path) in self.aliases():
            start_cmd.extend(['-c', 'Alias %s "%s"' % (alias, path)])

        if not port_obj.host.platform.is_win():
            start_cmd.extend(['-C', 'User "%s"' % os.environ.get("USERNAME", os.environ.get("USER", ""))])

        enable_ipv6 = self._port_obj.http_server_supports_ipv6()
        # Perform part of the checks Apache's APR does when trying to listen to
        # a specific host/port. This allows us to avoid trying to listen to
        # IPV6 addresses when it fails on Apache. APR itself tries to call
        # getaddrinfo() again without AI_ADDRCONFIG if the first call fails
        # with EBADFLAGS, but that is not how it normally fails in our use
        # cases, so ignore that for now.
        # See https://bugs.webkit.org/show_bug.cgi?id=98602#c7
        try:
            socket.getaddrinfo('::1', 0, 0, 0, 0, socket.AI_ADDRCONFIG)
        except:
            enable_ipv6 = False

        bind_address = '' if self._port_obj.get_option("http_all_interfaces") else '127.0.0.1:'

        for mapping in self._mappings:
            port = mapping['port']

            start_cmd += ['-C', 'Listen %s%d' % (bind_address, port)]

            # We listen to both IPv4 and IPv6 loop-back addresses, but ignore
            # requests to 8000 from random users on network.
            # See https://bugs.webkit.org/show_bug.cgi?id=37104
            if enable_ipv6:
                start_cmd += ['-C', 'Listen [::1]:%d' % port]

        if additional_dirs:
            for alias, path in additional_dirs.iteritems():
                start_cmd += ['-c', 'Alias %s "%s"' % (alias, path),
                        # Disable CGI handler for additional dirs.
                        '-c', '<Location %s>' % alias,
                        '-c', 'RemoveHandler .cgi .pl',
                        '-c', '</Location>']

        stop_cmd = [executable,
            '-f', config_file_path,
            '-c', 'PidFile "%s"' % self._pid_file,
            '-k', "stop"]

        start_cmd.extend(['-c', 'SSLCertificateFile "%s"' % cert_file])

        self._start_cmd = start_cmd
        self._stop_cmd = stop_cmd

    def _copy_apache_config_file(self, test_dir, output_dir):
        """Copy apache config file and returns the path to use.
        Args:
          test_dir: absolute path to the LayoutTests directory.
          output_dir: absolute path to the layout test results directory.
        """
        httpd_config = self._port_obj._path_to_apache_config_file()
        httpd_config_copy = os.path.join(output_dir, "httpd.conf")
        httpd_conf = self._filesystem.read_text_file(httpd_config)

        # FIXME: Why do we need to copy the config file since we're not modifying it?
        self._filesystem.write_text_file(httpd_config_copy, httpd_conf)

        if self.platform.is_cygwin():
            # Convert to MSDOS file naming:
            precompiledDrive = re.compile('^/cygdrive/[cC]')
            httpd_config_copy = precompiledDrive.sub("C:", httpd_config_copy)
            precompiledBuildbot = re.compile('^/home/buildbot')
            httpd_config_copy = precompiledBuildbot.sub("C:/cygwin/home/buildbot", httpd_config_copy)

        return httpd_config_copy

    @property
    def platform(self):
        return self._port_obj.host.platform

    def _spawn_process(self):
        _log.debug('Starting %s server, cmd="%s"' % (self._name, str(self._start_cmd)))
        retval, err = self._run(self._start_cmd)
        if retval or len(err):
            raise self._server_error('Failed to start %s' % self._name, err, retval)

        # For some reason apache isn't guaranteed to have created the pid file before
        # the process exits, so we wait a little while longer.
        if not self._wait_for_action(lambda: self._filesystem.exists(self._pid_file)):
            raise http_server_base.ServerError('Failed to start %s: no pid file found' % self._name)

        return int(self._filesystem.read_text_file(self._pid_file))

    def _stop_running_server(self):
        # If apache was forcefully killed, the pid file will not have been deleted, so check
        # that the process specified by the pid_file no longer exists before deleting the file.
        if self._pid and not self.platform.is_win() and not self._executive.check_running_pid(self._pid):
            self._filesystem.remove(self._pid_file)
            return

        retval, err = self._run(self._stop_cmd)

        # Windows httpd outputs shutdown status in stderr:
        if self.platform.is_win() and not retval and len(err):
            _log.debug('Shutdown: %s' % err)
            err = ""

        if retval or len(err):
            raise self._server_error('Failed to stop %s' % self._name, err, retval)

        # For some reason apache isn't guaranteed to have actually stopped after
        # the stop command returns, so we wait a little while longer for the
        # pid file to be removed.
        if not self._wait_for_action(lambda: not self._filesystem.exists(self._pid_file)):
            if self.platform.is_win():
                self._remove_pid_file()
                return

            raise http_server_base.ServerError('Failed to stop %s: pid file still exists' % self._name)

    def _run(self, cmd):
        process = self._executive.popen(cmd, stderr=self._executive.PIPE)
        process.wait()
        retval = process.returncode
        err = process.stderr.read()
        return (retval, err)

    def _server_error(self, message, stderr_output, exit_code):
        if self.platform.is_win() and exit_code == 720005 and not stderr_output:
            stderr_output = 'Access is denied. Do you have administrator privilege?'
        return http_server_base.ServerError('{}: {} (exit code={})'.format(message, stderr_output, exit_code))
