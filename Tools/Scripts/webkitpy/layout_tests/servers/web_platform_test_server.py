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

from webkitpy.layout_tests.servers import http_server_base

_log = logging.getLogger(__name__)


def doc_root(port_obj):
    doc_root = port_obj.get_option("wptserver_doc_root")
    if doc_root is None:
        return port_obj.host.filesystem.join("imported", "w3c", "web-platform-tests")
    return doc_root


def wpt_config_json(port_obj):
    config_wk_filepath = port_obj._filesystem.join(port_obj.layout_tests_dir(), doc_root(port_obj), "config.json")
    if not port_obj.host.filesystem.isfile(config_wk_filepath):
        return
    json_data = port_obj._filesystem.read_text_file(config_wk_filepath)
    return json.loads(json_data)


def base_http_url(port_obj, localhost_only=False):
    config = wpt_config_json(port_obj)
    if not config:
        # This should only be hit by webkitpy unit tests
        _log.debug("No WPT config file found")
        return "http://localhost:8800/"
    ports = config["ports"]
    host = config["browser_host"] if not localhost_only else "localhost"
    return "http://" + host + ":" + str(ports["http"][0]) + "/"


def base_https_url(port_obj, localhost_only=False):
    config = wpt_config_json(port_obj)
    if not config:
        # This should only be hit by webkitpy unit tests
        _log.debug("No WPT config file found")
        return "https://localhost:9443/"
    ports = config["ports"]
    host = config["browser_host"] if not localhost_only else "localhost"
    return "https://" + host + ":" + str(ports["https"][0]) + "/"


def base_url_list(port_obj):
    config = wpt_config_json(port_obj)
    host = config["browser_host"]
    plain_port = str(config["ports"]["http"][0])
    tls_port = str(config["ports"]["https"][0])

    urls = [
        "http://{}:{}/".format(host, plain_port),
        "https://{}:{}/".format(host, tls_port),
    ]
    # Some ports support aliases but this list is to be presented to users
    # so we include localhost which always will work in host browsers.
    if port_obj.supports_localhost_aliases and host not in ("127.0.0.1", "localhost"):
        urls += [
            "http://localhost:{}/".format(plain_port),
            "https://localhost:{}/".format(tls_port),
        ]

    return urls


def is_wpt_server_running(port_obj):
    config = wpt_config_json(port_obj)
    if not config:
        return False
    return http_server_base.HttpServerBase._is_running_on_port(config["ports"]["http"][0])


class WebPlatformTestServer(http_server_base.HttpServerBase):
    def __init__(self, port_obj, name, pidfile=None):
        http_server_base.HttpServerBase.__init__(self, port_obj)
        self._output_dir = port_obj.results_directory()

        self._name = name
        self._log_file_name = '%s_process_log.out.txt' % (self._name)

        self._output_log_path = None
        self._wsout = None
        self._process = None
        self._pid_file = pidfile
        if not self._pid_file:
            self._pid_file = self._filesystem.join(self._runtime_path, '%s.pid' % self._name)

        self._filesystem = port_obj.host.filesystem
        self._layout_root = port_obj.layout_tests_dir()
        self._doc_root = self._filesystem.join(self._layout_root, doc_root(port_obj))

        self._doc_root_path = self._filesystem.join(self._layout_root, self._doc_root)
        self._config_filename = self._filesystem.join(self._doc_root_path, "config.json")

        # FIXME https://webkit.org/b/222703
        python_interp = sys.executable
        if sys.version_info < (3, 0):
            python_interp = 'python3'

        wpt_file = self._filesystem.join(self._doc_root_path, "wpt.py")
        self._start_cmd = [python_interp, wpt_file, "serve", "--config", self._config_filename]

        self._mappings = []
        config = wpt_config_json(port_obj)
        if config:
            ports = config["ports"]
            for key in ports:
                for value in ports[key]:
                    port = {"port": value}
                    if key == "https":
                        port["sslcert"] = True
                    self._mappings.append(port)

    def ports_to_forward(self):
        return [mapping['port'] for mapping in self._mappings]

    def first_port(self, port_obj):
        config = wpt_config_json(port_obj)
        if not config:
            return None
        return config["ports"]["http"][0]

    def _prepare_config(self):
        self._filesystem.maybe_make_directory(self._output_dir)
        self._output_log_path = self._filesystem.join(self._output_dir, self._log_file_name)
        self._wsout = self._filesystem.open_text_file_for_writing(self._output_log_path)

        _log.debug('Copying WebKit web platform server config.json')
        config_wk_filename = self._filesystem.join(self._layout_root, "imported", "w3c", "resources", "config.json")
        if self._filesystem.isfile(config_wk_filename):
            config = json.loads(self._filesystem.read_text_file(config_wk_filename))
            if self._port_obj.supports_localhost_aliases and not self._port_obj.get_option('disable_wpt_hostname_aliases'):
                _log.debug('Using aliases to web-platform.test')
                config['browser_host'] = 'web-platform.test'
                config['alternate_hosts'] = {'alt': 'not-web-platform.test'}

            config['ssl']['openssl']['base_path'] = self._filesystem.join(self._output_dir, "_wpt_certs")
            self._filesystem.write_text_file(self._config_filename, json.dumps(config))

    def _spawn_process(self):
        self._process = self._executive.popen(self._start_cmd, cwd=self._doc_root_path, shell=False, stdin=self._executive.PIPE, stdout=self._wsout, stderr=self._wsout)
        self._filesystem.write_text_file(self._pid_file, str(self._process.pid))

        # Wait a second for the server to actually start so that tests do not start until server is running.
        time.sleep(1)

        # The server is not running after 1 second, something went wrong.
        if self._process.poll() is not None:
            self._stop_running_server()
            error_log = ('WPT Server process exited prematurely with status code %s\n' % self._process.returncode
                         + 'The cmdline for running the WPT server was: %s\n' % self._start_cmd
                         + 'The working dir was: %s\n' % self._doc_root_path)
            if self._output_log_path is not None and self._filesystem.exists(self._output_log_path):
                error_log += 'Check the logfile for the command at: %s\n' % self._output_log_path
            raise http_server_base.ServerError(error_log)

        return self._process.pid

    def _stop_running_server(self):
        _log.debug('Cleaning WPT web platform server config.json')
        if self._filesystem.isfile(self._config_filename):
            self._filesystem.remove(self._config_filename)

        if self._wsout:
            self._wsout.close()
            self._wsout = None

        if self._process is not None:
            self._process.poll()

        if self._pid:
            # kill_process will not kill the subprocesses, interrupt does the job.
            self._executive.interrupt(self._pid)

        self._remove_pid_file()
