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

"""Base class with common routines between the Apache and websocket servers."""

import errno
import json
import logging
import shutil
import socket
import subprocess
import sys
import tempfile
import time


_log = logging.getLogger(__name__)


class ServerError(Exception):
    pass


class HttpServerBase(object):
    """A skeleton class for starting and stopping servers used by the layout tests."""

    HTTP_SERVER_PORT = 8000
    ALTERNATIVE_HTTP_SERVER_PORT = 8080
    HTTPS_SERVER_PORT = 8443

    def __init__(self, port_obj):
        self._executive = port_obj._executive
        self._filesystem = port_obj._filesystem
        self._name = '<virtual>'
        self._mappings = {}
        self._pid = None
        self._pid_file = None
        self._port_obj = port_obj
        self.tests_dir = self._port_obj.layout_tests_dir()

        # We need a non-checkout-dependent place to put lock files, etc. We
        # don't use the Python default on the Mac because it defaults to a
        # randomly-generated directory under /var/folders and no one would ever
        # look there.
        tmpdir = tempfile.gettempdir()
        if port_obj.host.platform.is_mac():
            tmpdir = '/tmp'

        self._runtime_path = self._filesystem.join(tmpdir, "WebKit")
        self._filesystem.maybe_make_directory(self._runtime_path)

    def ports_to_forward(self):
        # Device port-forwarding is TCP-only; forwarding a UDP port (e.g. QUIC/HTTP-3) as TCP silently breaks it.
        return [mapping['port'] for mapping in self._mappings if not mapping.get('udp')]

    def start(self):
        """Starts the server. It is an error to start an already started server.

        This method also stops any stale servers started by a previous instance."""
        assert not self._pid, '%s server is already running' % self._name

        # Stop any stale servers left over from previous instances.
        if self._filesystem.exists(self._pid_file):
            try:
                self._pid = int(self._filesystem.read_text_file(self._pid_file))
                self._stop_running_server()
            except (ValueError, UnicodeDecodeError):
                # These could be raised if the pid file is corrupt.
                self._remove_pid_file()
            self._pid = None

        self._remove_stale_logs()
        self._prepare_config()
        self._check_that_all_ports_are_available()

        self._pid = self._spawn_process()

        if sys.platform == 'cygwin':
            # Starting the server takes longer time on Cygwin
            server_started = self._wait_for_action(self._is_server_running_on_all_ports, 60)
        else:
            server_started = self._wait_for_action(self._is_server_running_on_all_ports)

        if server_started:
            _log.debug("%s successfully started (pid = %d)" % (self._name, self._pid))
        else:
            self._stop_running_server()
            raise ServerError('Failed to start %s server' % self._name)

    def stop(self):
        """Stops the server. Stopping a server that isn't started is harmless."""
        actual_pid = None
        try:
            if self._filesystem.exists(self._pid_file):
                try:
                    actual_pid = int(self._filesystem.read_text_file(self._pid_file))
                except (ValueError, UnicodeDecodeError):
                    # These could be raised if the pid file is corrupt.
                    pass
                if not self._pid:
                    self._pid = actual_pid

            if not self._pid:
                return

            if not actual_pid:
                _log.warning('Failed to stop %s: pid file is missing' % self._name)
                return
            if self._pid != actual_pid:
                _log.warning('Failed to stop %s: pid file contains %d, not %d' %
                            (self._name, actual_pid, self._pid))
                # Try to kill the existing pid, anyway, in case it got orphaned.
                self._executive.kill_process(self._pid)
                self._pid = None
                return

            _log.debug("Attempting to shut down %s server at pid %d" % (self._name, self._pid))
            self._stop_running_server()
            _log.debug("%s server at pid %d stopped" % (self._name, self._pid))
            self._pid = None
        finally:
            # Make sure we delete the pid file no matter what happens.
            self._remove_pid_file()

    def _prepare_config(self):
        """This routine can be overridden by subclasses to do any sort
        of initialization required prior to starting the server that may fail."""
        pass

    def _remove_stale_logs(self):
        """This routine can be overridden by subclasses to try and remove logs
        left over from a prior run. This routine should log warnings if the
        files cannot be deleted, but should not fail unless failure to
        delete the logs will actually cause start() to fail."""
        pass

    def _spawn_process(self):
        """This routine must be implemented by subclasses to actually start the server.

        This routine returns the pid of the started process, and also ensures that that
        pid has been written to self._pid_file."""
        raise NotImplementedError()

    def _stop_running_server(self):
        """This routine must be implemented by subclasses to actually stop the running server listed in self._pid_file."""
        raise NotImplementedError()

    # Utility routines.

    def aliases(self):
        """Return path pairs used to define aliases. First item is URL path and second
        one is actual location in the file system."""
        json_data = self._filesystem.read_text_file(self._port_obj.path_from_webkit_base("Tools", "Scripts", "webkitpy", "layout_tests", "servers", "aliases.json"))
        return self._build_alias_path_pairs(json.loads(json_data))

    def _build_alias_path_pairs(self, data):
        def _make_path(path):
            return self._filesystem.join(self.tests_dir, self._filesystem.normpath(path))
        return [(alias, _make_path(path)) for (alias, path) in data]

    def _remove_pid_file(self):
        if self._filesystem.exists(self._pid_file):
            self._filesystem.remove(self._pid_file)

    def _remove_log_files(self, folder, starts_with):
        files = self._filesystem.listdir(folder)
        for file in files:
            if file.startswith(starts_with):
                full_path = self._filesystem.join(folder, file)
                self._filesystem.remove(full_path)

    def _wait_for_action(self, action, wait_secs=20.0, sleep_secs=0.1):
        """Repeat the action for wait_sec or until it succeeds, sleeping for sleep_secs
        in between each attempt. Returns whether it succeeded."""
        start_time = time.time()
        while time.time() - start_time < wait_secs:
            if action():
                return True
            _log.debug("Waiting for action: %s" % action)
            time.sleep(sleep_secs)

        return False

    def _is_server_running_on_all_ports(self):
        """Returns whether the server is running on all the desired ports."""
        if not self._port_obj.host.platform.is_win() and not self._executive.check_running_pid(self._pid):
            _log.debug("Server isn't running at all")
            raise ServerError("Server exited")

        for mapping in self._mappings:
            if mapping.get('udp'):
                if not self._is_udp_port_listening(mapping['port'], owner_pid=self._pid):
                    return False
            elif not self._is_running_on_port(mapping['port']):
                return False
        return True

    @classmethod
    def _is_running_on_port(cls, port):
        s = socket.socket()
        try:
            s.connect(('localhost', port))
            _log.debug("Server running on %d" % port)
        except IOError as e:
            if e.errno not in (errno.ECONNREFUSED, errno.ECONNRESET):
                raise
            _log.debug("Server NOT running on %d: %s" % (port, e))
            return False
        finally:
            s.close()
        return True

    @classmethod
    def _is_udp_port_listening(cls, port, owner_pid=None):
        # No UDP equivalent of TCP connect(): the server holds the port bound, so a bind() to its address
        # (127.0.0.1) failing with EADDRINUSE means "listening".
        s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        try:
            s.bind(('127.0.0.1', port))
        except OSError as e:
            if e.errno != errno.EADDRINUSE:
                raise
            if owner_pid is None:
                return True
            # Accept the port unless every holder is known to belong to another process tree.
            holders = cls.processes_holding_port(port, is_udp=True)
            if not holders:
                return True
            parents = cls._parent_pids()
            if any(cls._is_pid_or_descendant(pid, owner_pid, parents) is not False
                   for pid, _ in holders):
                return True
            _log.debug('UDP port %d is held by %s, none of which belongs to pid %s' % (
                port, ', '.join('%s (pid %s)' % (command, pid) for pid, command in holders),
                owner_pid))
            return False
        finally:
            s.close()
        return False

    @staticmethod
    def _parent_pids():
        try:
            completed = subprocess.run(['/bin/ps', '-A', '-o', 'pid=,ppid='],
                                       capture_output=True, text=True, timeout=5)
        except (OSError, subprocess.SubprocessError):
            return None
        if completed.returncode:
            return None
        parents = {}
        for line in completed.stdout.splitlines():
            fields = line.split()
            if len(fields) != 2:
                continue
            try:
                parents[int(fields[0])] = int(fields[1])
            except ValueError:
                continue
        return parents or None

    @staticmethod
    def _is_pid_or_descendant(pid, ancestor_pid, parents):
        try:
            current = int(pid)
            ancestor = int(ancestor_pid)
        except (TypeError, ValueError):
            return None
        if current == ancestor:
            return True
        if parents is None:
            return None
        seen = set()
        while current > 1 and current not in seen:
            seen.add(current)
            current = parents.get(current)
            if current is None:
                return False
            if current == ancestor:
                return True
        return False

    def _check_that_all_ports_are_available(self):
        for mapping in self._mappings:
            port = mapping['port']
            is_udp = bool(mapping.get('udp'))
            s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM if is_udp else socket.SOCK_STREAM)
            if not is_udp:
                s.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
            try:
                s.bind(('127.0.0.1' if is_udp else 'localhost', port))
            except IOError as e:
                if e.errno in (errno.EALREADY, errno.EADDRINUSE):
                    raise ServerError(self.port_in_use_message(port, is_udp=is_udp))
                elif sys.platform.startswith('win') and e.errno in (errno.WSAEACCES,):  # pylint: disable=E1101
                    raise ServerError(self.port_in_use_message(port, is_udp=is_udp))
                else:
                    raise
            finally:
                s.close()

    @staticmethod
    def processes_holding_port(port, is_udp=False):
        lsof = shutil.which('lsof') or shutil.which('lsof', path='/usr/sbin')
        if not lsof:
            return []
        command = [lsof, '-nP', '-F', 'pc', '-i{}:{}'.format('UDP' if is_udp else 'TCP', port)]
        if not is_udp:
            command.append('-sTCP:LISTEN')
        try:
            output = subprocess.run(command, capture_output=True, text=True, timeout=5).stdout
        except (OSError, subprocess.SubprocessError):
            return []
        holders = []
        pid = None
        for line in output.splitlines():
            if line.startswith('p'):
                pid = line[1:]
            elif line.startswith('c') and pid:
                holders.append((pid, line[1:]))
                pid = None
        return holders

    @classmethod
    def port_in_use_message(cls, port, is_udp=False):
        message = '{} port {} is already in use.'.format('UDP' if is_udp else 'TCP', port)
        holders = cls.processes_holding_port(port, is_udp=is_udp)
        if holders:
            message += ' Held by {}.'.format(
                ', '.join('{} (pid {})'.format(command, pid) for pid, command in holders))
        return message


def is_http_server_running():
    return HttpServerBase._is_running_on_port(HttpServerBase.HTTP_SERVER_PORT)
