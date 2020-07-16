#!/usr/bin/env python

import logging
import os
import re
import subprocess
import sys
import time

from webkitpy.benchmark_runner.http_server_driver.http_server_driver import HTTPServerDriver


_log = logging.getLogger(__name__)


class SimpleHTTPServerDriver(HTTPServerDriver):

    """This class depends on unix environment, need to be modified to achieve crossplatform compability
    """

    platforms = ['osx', 'linux']

    def __init__(self):
        self._server_process = None
        self._server_port = 0
        self._ip = '127.0.0.1'
        self._ensure_http_server_dependencies()

    def serve(self, web_root):
        _log.info('Launching an http server')
        http_server_path = os.path.join(os.path.dirname(os.path.abspath(__file__)), "http_server/twisted_http_server.py")
        interface_args = []
        if self._ip:
            interface_args.extend(['--interface', self._ip])
        self._server_process = subprocess.Popen(["python", http_server_path, web_root] + interface_args, stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.PIPE)

        max_attempt = 5
        interval = 0.5
        _log.info('Start to fetching the port number of the http server')
        try:
            import psutil
            for attempt in range(max_attempt):
                connections = psutil.Process(self._server_process.pid).connections()
                if connections and connections[0].laddr and connections[0].laddr[1] and connections[0].status == 'LISTEN':
                    self._server_port = connections[0].laddr[1]
                    _log.info('HTTP Server is serving at port: %d', self._server_port)
                    break
                _log.info('Server port is not found this time, retry after %f seconds' % interval)
                time.sleep(interval)
                interval *= 2
            else:
                raise Exception("Server is not listening on port, max tries exceeded. HTTP server may be installing dependent modules.")
        except ImportError:
            for attempt in range(max_attempt):
                try:
                    output = subprocess.check_output(['/usr/sbin/lsof', '-a', '-P', '-iTCP', '-sTCP:LISTEN', '-p', str(self._server_process.pid)])
                    self._server_port = int(re.search('TCP .*:(\d+) \(LISTEN\)', output).group(1))
                    if self._server_port:
                        _log.info('HTTP Server is serving at port: %d', self._server_port)
                        break
                except Exception as error:
                    _log.info('Error: %s' % error)
                _log.info('Server port is not found this time, retry after %f seconds' % interval)
                time.sleep(interval)
                interval *= 2
            else:
                raise Exception("Cannot listen to server, max tries exceeded")
        self._wait_for_http_server()

    def _wait_for_http_server(self):
        max_attempt = 5
        # Wait for server to be up completely before exiting
        for attempt in range(max_attempt):
            try:
                subprocess.check_call(["curl", "--silent", "--head", "--fail", "--output", "/dev/null", self.base_url()])
                return
            except Exception as error:
                _log.info('Server not running yet: %s' % error)
                time.sleep(1)
        raise Exception('Server not running, max tries exceeded: %s' % error)

    def base_url(self):
        return "http://%s:%d" % (self._ip, self._server_port)

    def fetch_result(self):
        (stdout, stderr) = self._server_process.communicate()
        print(stderr)
        return stdout

    def kill_server(self):
        try:
            if not self._server_process:
                return
            if self._server_process.poll() is None:
                self._server_process.terminate()
        except OSError as error:
            _log.info('Error terminating server process: %s' % (error))

    def get_return_code(self):
        return self._server_process.returncode

    def set_device_id(self, device_id):
        pass

    def _ensure_http_server_dependencies(self):
        _log.info('Ensure dependencies of http server is satisfied')
        from pkg_resources import require, VersionConflict, DistributionNotFound
        try:
            require("Twisted>=15.5.0")
            import twisted
        except (ImportError, VersionConflict, DistributionNotFound):
            _log.info("Will install twisted in webkitpy, and twisted will be used by webkitpy only")
            sys.path.append(os.path.abspath(os.path.join(os.path.dirname(os.path.abspath(__file__)), '../../../..')))
            from webkitpy.thirdparty.autoinstalled.twisted_15_5_0 import twisted
