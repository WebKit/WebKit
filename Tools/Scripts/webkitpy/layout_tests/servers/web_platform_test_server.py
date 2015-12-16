#  Copyright (c) 2014, Canon Inc. All rights reserved.
#  Redistribution and use in source and binary forms, with or without
#  modification, are permitted provided that the following conditions
#  are met:
#  1.  Redistributions of source code must retain the above copyright
#      notice, this list of conditions and the following disclaimer.
#  2.  Redistributions in binary form must reproduce the above copyright
#      notice, this list of conditions and the following disclaimer in the
#      documentation and/or other materials provided with the distribution.
#  3.  Neither the name of Canon Inc. nor the names of
#      its contributors may be used to endorse or promote products derived
#      from this software without specific prior written permission.
#  THIS SOFTWARE IS PROVIDED BY CANON INC. AND ITS CONTRIBUTORS "AS IS" AND ANY
#  EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
#  WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
#  DISCLAIMED. IN NO EVENT SHALL CANON INC. AND ITS CONTRIBUTORS BE LIABLE FOR
#  ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
#  DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
#  SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
#  CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
#  OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
#  OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

import json
import logging
import sys
import time

from webkitpy.common.system.autoinstall import AutoInstaller
from webkitpy.layout_tests.servers import http_server_base

_log = logging.getLogger(__name__)


def doc_root(port_obj):
    doc_root = port_obj.get_option("wptserver_doc_root")
    if doc_root is None:
        return port_obj.host.filesystem.join("imported", "w3c", "web-platform-tests")
    return doc_root


def base_url(port_obj):
    config_wk_filepath = port_obj._filesystem.join(port_obj.layout_tests_dir(), "imported", "w3c", "resources", "config.json")
    if not port_obj.host.filesystem.isfile(config_wk_filepath):
        # This should only be hit by webkitpy unit tests
        _log.debug("No WPT config file found")
        return "http://localhost:8800/"
    json_data = port_obj._filesystem.read_text_file(config_wk_filepath)
    config = json.loads(json_data)
    ports = config["ports"]
    return "http://" + config["host"] + ":" + str(ports["http"][0]) + "/"


class WebPlatformTestServer(http_server_base.HttpServerBase):
    def __init__(self, port_obj, name, pidfile=None):
        http_server_base.HttpServerBase.__init__(self, port_obj)
        self._output_dir = port_obj.results_directory()

        self._name = name
        self._log_file_name = '%s_process_log.out.txt' % (self._name)

        self._wsout = None
        self._process = None
        self._pid_file = pidfile
        if not self._pid_file:
            self._pid_file = self._filesystem.join(self._runtime_path, '%s.pid' % self._name)
        self._servers_file = self._filesystem.join(self._runtime_path, '%s_servers.json' % (self._name))

        self._stdout_data = None
        self._stderr_data = None
        self._filesystem = port_obj.host.filesystem
        self._layout_root = port_obj.layout_tests_dir()
        self._doc_root = self._filesystem.join(self._layout_root, doc_root(port_obj))

        self._resources_files_to_copy = ['testharness.css', 'testharnessreport.js']

        current_dir_path = self._filesystem.abspath(self._filesystem.split(__file__)[0])
        self._start_cmd = ["python", self._filesystem.join(current_dir_path, "web_platform_test_launcher.py"), self._servers_file]
        self._doc_root_path = self._filesystem.join(self._layout_root, self._doc_root)

    def _install_modules(self):
        modules_file_path = self._filesystem.join(self._doc_root_path, "..", "resources", "web-platform-tests-modules.json")
        if not self._filesystem.isfile(modules_file_path):
            _log.warning("Cannot read " + modules_file_path)
            return
        modules = json.loads(self._filesystem.read_text_file(modules_file_path))
        for module in modules:
            AutoInstaller(target_dir=self._filesystem.join(self._doc_root, self._filesystem.sep.join(module["path"]))).install(url=module["url"], url_subpath=module["url_subpath"], target_name=module["name"])

    def _copy_webkit_test_files(self):
        _log.debug('Copying WebKit resources files')
        for f in self._resources_files_to_copy:
            webkit_filename = self._filesystem.join(self._layout_root, "resources", f)
            if self._filesystem.isfile(webkit_filename):
                self._filesystem.copyfile(webkit_filename, self._filesystem.join(self._doc_root, "resources", f))
        _log.debug('Copying WebKit web platform server config.json')
        config_wk_filename = self._filesystem.join(self._layout_root, "imported", "w3c", "resources", "config.json")
        if self._filesystem.isfile(config_wk_filename):
            config_json = self._filesystem.read_text_file(config_wk_filename).replace("%CERTS_DIR%", self._filesystem.join(self._output_dir, "_wpt_certs"))
            self._filesystem.write_text_file(self._filesystem.join(self._doc_root, "config.json"), config_json)

        wpt_testharnessjs_file = self._filesystem.join(self._doc_root, "resources", "testharness.js")
        layout_tests_testharnessjs_file = self._filesystem.join(self._layout_root, "resources", "testharness.js")
        if (not self._filesystem.compare(wpt_testharnessjs_file, layout_tests_testharnessjs_file)):
            _log.warning("\n//////////\nWPT tests are not using the same testharness.js file as other WebKit Layout tests.\nWebKit testharness.js might need to be updated according WPT testharness.js.\n//////////\n")

    def _clean_webkit_test_files(self):
        _log.debug('Cleaning WPT resources files')
        for f in self._resources_files_to_copy:
            wpt_filename = self._filesystem.join(self._doc_root, "resources", f)
            if self._filesystem.isfile(wpt_filename):
                self._filesystem.remove(wpt_filename)
        _log.debug('Cleaning WPT web platform server config.json')
        config_wpt_filename = self._filesystem.join(self._doc_root, "config.json")
        if self._filesystem.isfile(config_wpt_filename):
            self._filesystem.remove(config_wpt_filename)

    def _prepare_config(self):
        if self._filesystem.exists(self._output_dir):
            output_log = self._filesystem.join(self._output_dir, self._log_file_name)
            self._wsout = self._filesystem.open_text_file_for_writing(output_log)
        self._install_modules()
        self._copy_webkit_test_files()

    def _spawn_process(self):
        self._stdout_data = None
        self._stderr_data = None
        if self._wsout:
            self._process = self._executive.popen(self._start_cmd, cwd=self._doc_root_path, shell=False, stdin=self._executive.PIPE, stdout=self._wsout, stderr=self._wsout)
        else:
            self._process = self._executive.popen(self._start_cmd, cwd=self._doc_root_path, shell=False, stdin=self._executive.PIPE, stdout=self._executive.PIPE, stderr=self._executive.STDOUT)
        self._filesystem.write_text_file(self._pid_file, str(self._process.pid))

        # Wait a second for the server to actually start so that tests do not start until server is running.
        time.sleep(1)

        return self._process.pid

    def _stop_running_subservers(self):
        if self._filesystem.exists(self._servers_file):
            try:
                json_data = self._filesystem.read_text_file(self._servers_file)
                started_servers = json.loads(json_data)
                for server in started_servers:
                    if self._executive.check_running_pid(server['pid']):
                        _log.warning('Killing server process (protocol: %s , port: %d, pid: %d).' % (server['protocol'], server['port'], server['pid']))
                        self._executive.kill_process(server['pid'])
            finally:
                self._filesystem.remove(self._servers_file)

    def stop(self):
        super(WebPlatformTestServer, self).stop()
        # In case of orphaned pid, kill the running subservers if any still alive.
        self._stop_running_subservers()

    def _stop_running_server(self):
        _log.debug('Stopping %s server' % (self._name))
        self._clean_webkit_test_files()

        if self._process:
            (self._stdout_data, self._stderr_data) = self._process.communicate(input='\n')
        if self._wsout:
            self._wsout.close()
            self._wsout = None

        if self._pid and self._executive.check_running_pid(self._pid):
            _log.warning('Cannot stop %s server normally.' % (self._name))
            _log.warning('Killing server launcher process (pid: %d).' % (self._pid))
            self._executive.kill_process(self._pid)

        self._remove_pid_file()
        self._stop_running_subservers()
