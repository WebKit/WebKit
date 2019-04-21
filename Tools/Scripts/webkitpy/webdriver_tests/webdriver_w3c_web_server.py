# Copyright (C) 2017 Igalia S.L.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1.  Redistributions of source code must retain the above copyright
#     notice, this list of conditions and the following disclaimer.
# 2.  Redistributions in binary form must reproduce the above copyright
#     notice, this list of conditions and the following disclaimer in the
#     documentation and/or other materials provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND ANY
# EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
# WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
# DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR ANY
# DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
# (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
# LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
# ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
# SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

import errno
import json
import logging
import os
import socket
import tempfile
import time

from webkitpy.common.webkit_finder import WebKitFinder

_log = logging.getLogger(__name__)


class WebDriverW3CWebServer(object):

    def __init__(self, port):
        self._port = port
        self._name = "wptwd"

        layout_root = self._port.layout_tests_dir()
        self._layout_doc_root = os.path.join(layout_root, 'imported', 'w3c', 'web-platform-tests')
        self._process = None
        self._pid = None
        self._wsout = None

        tmpdir = tempfile.gettempdir()
        if self._port.host.platform.is_mac():
            tmpdir = '/tmp'
        self._runtime_path = os.path.join(tmpdir, "WebKitWebDriverTests")
        self._port.host.filesystem.maybe_make_directory(self._runtime_path)

        self._pid_file = os.path.join(self._runtime_path, '%s.pid' % self._name)

        # FIXME: We use the runtime path for now for log output, since we don't have a results directory yet.
        self._output_log_path = os.path.join(self._runtime_path, '%s_process_log.out.txt' % (self._name))

    def _wait_for_server(self, wait_secs=20, sleep_secs=1):
        def check_port(host, port):
            s = socket.socket()
            try:
                s.connect((host, port))
            except IOError as e:
                if e.errno not in (errno.ECONNREFUSED, errno.ECONNRESET):
                    raise
                return False
            finally:
                s.close()
            return True

        start_time = time.time()
        while time.time() - start_time < wait_secs:
            if self._port._executive.check_running_pid(self._pid) and check_port(self._server_host, self._server_port):
                return True
            time.sleep(sleep_secs)
        return False

    def start(self):
        assert not self._pid, '%s server is already running' % self._name

        # Stop any stale servers left over from previous instances.
        if self._port.host.filesystem.exists(self._pid_file):
            try:
                self._pid = int(self._port.host.filesystem.read_text_file(self._pid_file))
                self.stop()
            except (ValueError, UnicodeDecodeError):
                # These could be raised if the pid file is corrupt.
                self._port.host.filesystem.remove(self._pid_file)
                self._pid = None

        _log.debug('Copying WebDriver WPT server config.json')
        doc_root = os.path.join(WebKitFinder(self._port.host.filesystem).path_from_webkit_base('WebDriverTests'), 'imported', 'w3c')
        config_filename = os.path.join(doc_root, 'config.json')
        config_json = self._port.host.filesystem.read_text_file(config_filename).replace("%DOC_ROOT%", doc_root)
        self._port.host.filesystem.write_text_file(os.path.join(self._layout_doc_root, 'config.json'), config_json)
        config = json.loads(config_json)
        self._server_host = config['browser_host']
        self._server_port = config['ports']['http'][0]

        self._wsout = self._port.host.filesystem.open_text_file_for_writing(self._output_log_path)
        wpt_file = os.path.join(self._layout_doc_root, "wpt.py")
        cmd = ["python", wpt_file, "serve", "--config", os.path.join(self._layout_doc_root, 'config.json')]
        self._process = self._port._executive.popen(cmd, cwd=self._layout_doc_root, shell=False, stdin=self._port._executive.PIPE, stdout=self._wsout, stderr=self._wsout)
        self._pid = self._process.pid
        self._port.host.filesystem.write_text_file(self._pid_file, str(self._pid))

        if not self._wait_for_server():
            _log.error('WPT Server process exited prematurely with status code %s' % self._process.returncode)
            self.stop()
            raise RuntimeError

        _log.info('WebDriver WPT server listening at http://%s:%s/' % (self._server_host, self._server_port))

    def stop(self):
        _log.debug('Cleaning WebDriver WPT server config.json')
        temporary_config_file = os.path.join(self._layout_doc_root, 'config.json')
        if self._port.host.filesystem.exists(temporary_config_file):
            self._port.host.filesystem.remove(temporary_config_file)
        if self._wsout:
            self._wsout.close()
            self._wsout = None
        if self._pid:
            # kill_process will not kill the subprocesses, interrupt does the job.
            self._port._executive.interrupt(self._pid)
        self._port.host.filesystem.remove(self._pid_file)
        self._pid = None

    def host(self):
        return self._server_host

    def port(self):
        return self._server_port

    def document_root(self):
        return self._layout_doc_root

    # Waits indefinitely until the webserver process is terminated.
    def wait(self):
        if not self._pid:
            return

        self._process.wait()

    def __enter__(self):
        if not self._pid:
            self.start()

        return self

    def __exit__(self, exc_type, exc_value, exc_traceback):
        if self._pid:
            self.stop()
