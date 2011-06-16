#!/usr/bin/env python
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

"""Base class with common routines between the Apache, Lighttpd, and websocket servers."""

import errno
import logging
import socket
import sys
import tempfile
import time

_log = logging.getLogger("webkitpy.layout_tests.port.http_server_base")


class ServerError(Exception):
    pass


class HttpServerBase(object):
    """A skeleton class for starting and stopping servers used by the layout tests."""

    def __init__(self, port_obj):
        self._executive = port_obj._executive
        self._filesystem = port_obj._filesystem
        self._name = '<virtual>'
        self._mappings = {}
        self._pid_file = None
        self._port_obj = port_obj
        self._process = None

        # We need a non-checkout-dependent place to put lock files, etc. We
        # don't use the Python default on the Mac because it defaults to a
        # randomly-generated directory under /var/folders and no one would ever
        # look there.
        tmpdir = tempfile.gettempdir()
        if sys.platform == 'darwin':
            tmpdir = '/tmp'

        self._runtime_path = self._filesystem.join(tmpdir, "WebKit")
        port_obj.maybe_make_directory(self._runtime_path)

    def start(self):
        """Starts the server."""
        assert not self._process, '%s server is already running.'

        self._remove_stale_processes()
        self._remove_stale_logs()
        self._prepare_config()
        self._check_that_all_ports_are_available()
        self._process = self._spawn_process()
        _log.debug("%s started. Testing ports" % self._name)
        server_started = self._wait_for_action(self._is_server_running_on_all_ports)
        if server_started:
            _log.debug("%s successfully started" % self._name)
        else:
            raise ServerError('Failed to start %s server' % self._name)

        if self._pid_file:
            self._filesystem.write_text_file(self._pid_file, str(self._process.pid))

    def stop(self):
        _log.debug("Shutting down any running %s servers" % self._name)
        pid = None
        if self._process:
            pid = self._process.pid
            _log.debug("Attempting to shut down %s server at pid %d" % (self._name, pid))
        elif self._pid_file and self._filesystem.exists(self._pid_file):
            pid = int(self._filesystem.read_text_file(self._pid_file))
        else:
            _log.warning("No running %s servers found" % self._name)
            return

        self._port_obj._executive.kill_process(pid)
        if self._process:
            # wait() is not threadsafe and can throw OSError due to:
            # http://bugs.python.org/issue1731717
            try:
                self._process.wait()
            except OSError, e:
                if e.errno != errno.ECHILD:
                    raise
            self._process = None
        if self._pid_file and self._filesystem.exists(self._pid_file):
            self._filesystem.remove(self._pid_file)
        _log.debug("%s server at pid %d killed" % (self._name, pid))

        self._cleanup_after_stop()

    def _prepare_config(self):
        """This routine should be implemented by subclasses to do any sort
        of initialization required prior to starting the server that may fail."""
        pass

    def _remove_stale_processes(self):
        """Remove any instances of the server left over from a previous run.
        There should be at most one of these because of the global http lock."""
        def kill_and_check(name, pid):
            _log.warning('Previously started %s is still running as pid %d, trying to kill it.' % (name, pid))
            self._executive.kill_process(pid)
            return not self._executive.check_running_pid(pid)

        if self._pid_file and self._filesystem.exists(self._pid_file):
            pid = int(self._filesystem.read_text_file(self._pid_file))
            if not self._executive.check_running_pid(pid) or self._wait_for_action(lambda: kill_and_check(self._name, pid)):
                self._filesystem.remove(self._pid_file)

    def _remove_stale_logs(self):
        """This routine should be implemented by subclasses to try and remove logs
        left over from a prior run. This routine should log warnings if the
        files cannot be deleted, but should not fail unless failure to
        delete the logs will actually cause start() to fail."""
        pass

    def _spawn_process(self):
        """This routine mst be implemented by subclasses to actually
        launch the subprocess and return the process object. The method
        does not need to ensure that the process started correctly (start()
        will verify that itself)."""
        raise NotImplementedError

    def _cleanup_after_stop(self):
        """This routine may be implemented in subclasses to clean up any state
        as necessary after the server process is stopped."""
        pass

    def _remove_log_files(self, folder, starts_with):
        files = self._filesystem.listdir(folder)
        for file in files:
            if file.startswith(starts_with):
                full_path = self._filesystem.join(folder, file)
                self._filesystem.remove(full_path)

    def _wait_for_action(self, action):
        """Repeat the action for 20 seconds or until it succeeds. Returns
        whether it succeeded."""
        start_time = time.time()
        while time.time() - start_time < 20:
            if action():
                return True
            _log.debug("Waiting for action: %s" % action)
            time.sleep(1)

        return False

    def _is_server_running_on_all_ports(self):
        """Returns whether the server is running on all the desired ports."""
        if not self._executive.check_running_pid(self._process.pid):
            _log.debug("Server isn't running at all")
            raise ServerError("Server exited")

        for mapping in self._mappings:
            s = socket.socket()
            port = mapping['port']
            try:
                s.connect(('localhost', port))
                _log.debug("Server running on %d" % port)
            except IOError, e:
                if e.errno not in (errno.ECONNREFUSED, errno.ECONNRESET):
                    raise
                _log.debug("Server NOT running on %d: %s" % (port, e))
                return False
            finally:
                s.close()
        return True

    def _check_that_all_ports_are_available(self):
        for mapping in self._mappings:
            s = socket.socket()
            port = mapping['port']
            try:
                s.bind(('localhost', port))
            except IOError, e:
                if e.errno in (errno.EALREADY, errno.EADDRINUSE):
                    raise ServerError('Port %d is already in use.' % port)
                else:
                    raise
            finally:
                s.close()
